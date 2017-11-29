/*
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	if(flags == allflags)//check for valid falgs
		return EINVAL;

	char *kpath = (char*)kmalloc(sizeof(char)*PATH_MAX);
	struct openfile *file;
	int result = 0;
	//copy in supplied pathname
	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	if(result != 0) 
		return EFAULT;
	//open the file
	result = openfile_open(kpath, flags, mode, &file);
	if(result != 0)
		return EFAULT;
	//place the file into curproc's file table
	result = filetable_place(curproc->p_filetable, file, retval);
	if(result != 0)
		return EMFILE;

	kfree(kpath);

	/* 
	 * Your implementation of system call open starts here.  
	 *
	 * Check the design document design/filesyscall.txt for the steps
	 */
	//(void) upath; // suppress compilation warning until code gets written
	//(void) flags; // suppress compilation warning until code gets written
	//(void) mode; // suppress compilation warning until code gets written
	//(void) retval; // suppress compilation warning until code gets written
	//(void) allflags; // suppress compilation warning until code gets written
	//(void) kpath; // suppress compilation warning until code gets written
	//(void) file; // suppress compilation warning until code gets written

	return result;
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	int result = 0;
	struct openfile *openFile;
	struct iovec ioVEC;
	struct uio uIO;
	retval = 0;
	//translate fd to open file object
	result = filetable_get(curproc->p_filetable, fd, &openFile);
	//checking if filetable was gotten
	if(result != 0)
		return result;
	//lock seekable object
	lock_acquire(openFile->of_offsetlock);
	//check for files opened write-only
	if(openFile->of_accmode == O_WRONLY)
	{	//release lock because can't read a write only file
		lock_release(openFile->of_offsetlock);
		return EACCES;
	}
	//construct a struct uio
	uio_kinit(&ioVEC, &uIO, buf, size, openFile->of_offset, UIO_READ);
	//call VOP_READ
	result = VOP_READ(openFile->of_vnode, &uIO);
	if(result != 0)
		return result;

	//update seek position
	openFile->of_offset = uIO.uio_offset;	
	//unlock
	lock_release(openFile->of_offsetlock);
	//filetable)put
	filetable_put(curproc->p_filetable, fd, openFile);
	//set return value
	//*retval = size - uIO.uio_resid;
	result = 0;
	
	

       /* 
        * Your implementation of system call read starts here.  
        *
        * Check the design document design/filesyscall.txt for the steps
        */
     //  (void) fd; // suppress compilation warning until code gets written
      // (void) buf; // suppress compilation warning until code gets written
      // (void) size; // suppress compilation warning until code gets written
       (void) retval; // suppress compilation warning until code gets written

       return result;
}

int sys_write(int fd, userptr_t buf, size_t size, int *retval)
{
	int result = 0;
	struct openfile *openFile;
	struct iovec ioVEC;
	struct uio uIO;
	retval = 0;
	
	//translate fd to open file object
	result = filetable_get(curproc->p_filetable, fd, &openFile);
	//checking if filetable was gotten
	if(result != 0)
		return result;
	//lock seekable object
	lock_acquire(openFile->of_offsetlock);
	//check for files opened read-only
	if(openFile->of_accmode == O_RDONLY)
	{	//release lock because can't write a read only file
		lock_release(openFile->of_offsetlock);
		return EACCES;
	}
	//construct a struct uio
	uio_kinit(&ioVEC, &uIO, buf, size, openFile->of_offset, UIO_WRITE);
	//call VOP_WRITE
	result = VOP_WRITE(openFile->of_vnode, &uIO);
	if(result != 0)
		return result;

	//update seek position
	openFile->of_offset = uIO.uio_offset;	
	//unlock
	lock_release(openFile->of_offsetlock);
	//filetable)put
	filetable_put(curproc->p_filetable, fd, openFile);
	//set return value
	//*retval = size - uIO.uio_resid;
	//result = 0;
	(void) retval;
	return result;
}
/*
 * write() - write data to a file
 */

int sys_close(int fd)
{
	struct openfile *openFile;
	//validate the fd number	
	KASSERT(filetable_okfd(curproc->p_filetable, fd));
	//replace curproc's file table entry with NULL	
	filetable_placeat(curproc->p_filetable, NULL, fd, &openFile);
	//check if file was open
	if(openFile == NULL)
		return ENOENT;
	else //decref the open file
		openfile_decref(openFile);

	return 0;
	
}
/*
 * close() - remove from the file table.
 */
int sys_meld(const_userptr_t pn1, const_userptr_t pn2, const_userptr_t pn3, int *retval)
{
	struct openfile *openFile1, *openFile2, *openFile3;

	int result = 0;
	char *kpath_pn1 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	char *kpath_pn2 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	char *kpath_pn3 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	int counter = 0;
	int fd;
	int tempVal;

	//copy in supplied pathnames
	result = copyinstr(pn1, kpath_pn1, PATH_MAX, NULL);
	if(result != 0)
		return result;
	result = copyinstr(pn2, kpath_pn2, PATH_MAX, NULL);
	if(result != 0)
		return result;
	result = copyinstr(pn3, kpath_pn3, PATH_MAX, NULL);
	if(result != 0)
		return result;

	//open the first two files for reading

	result = openfile_open(kpath_pn1, O_RDWR, 0664, &openFile1);
	if(result != 0)
		return ENOENT;
	result = openfile_open(kpath_pn2, O_RDWR, 0664, &openFile2);
	if(result != 0)
		return ENOENT;

	//open the third file for writing
	result = openfile_open(kpath_pn3, O_WRONLY | O_CREAT | O_EXCL, 0664, &openFile3);
	if(result != 0)
		return EEXIST;
	
	//place them in filetable 
	result = filetable_place(curproc->p_filetable, openFile1, &tempVal);
	if(result != 0)
		return result;
	result = filetable_place(curproc->p_filetable, openFile2, &tempVal);
	if(result != 0)
		return result;
	result = filetable_place(curproc->p_filetable, openFile3, &tempVal);
	if(result != 0)
		return result;
	fd = tempVal;
	
	//buffers for 4 bytes of each file
	char *buffer1 = (char*)kmalloc(sizeof(char)*4);
	char *buffer2 = (char*)kmalloc(sizeof(char)*4);

	struct stat st;
	//size of first file
	result = VOP_STAT(openFile1->of_vnode, &st);
	int size = st.st_size;
	//size of second file
	result = VOP_STAT(openFile2->of_vnode, &st);
	size = size + st.st_size;

	struct iovec ioVec;
	struct uio uIO1, uIO2, wUIO;

	while(counter < size/2){
	//file 1 reading 4 bytes
		lock_acquire(openFile1->of_offsetlock);
		
		uio_kinit(&ioVec, &uIO1, buffer1, 4, openFile1->of_offset, UIO_READ);
		result = VOP_READ(openFile1->of_vnode, &uIO1);
		if(result != 0)
			return result;
		openFile1->of_offset = uIO1.uio_offset;

		lock_release(openFile1->of_offsetlock);
		

		//file 2 reading 4 bytes
		lock_acquire(openFile2->of_offsetlock);
		uio_kinit(&ioVec, &uIO2, buffer2, 4, openFile2->of_offset, UIO_READ);
		result = VOP_READ(openFile2->of_vnode, &uIO2);
		if(result != 0)
			return result;
		openFile2->of_offset = uIO2.uio_offset;
		
		lock_release(openFile2->of_offsetlock);

		//write 4 bytes from file 1 to the meld file
		lock_acquire(openFile3->of_offsetlock);
		uio_kinit(&ioVec, &wUIO, buffer1, 4,openFile3->of_offset, UIO_WRITE);
		result = VOP_WRITE(openFile3->of_vnode, &wUIO);
		if(result != 0)
			return result;
		openFile3->of_offset = wUIO.uio_offset;
		
		lock_release(openFile3->of_offsetlock);

		
		//write 4 bytes from file 2 to the meld file
		lock_acquire(openFile3->of_offsetlock);
		uio_kinit(&ioVec, &wUIO, buffer2, 4, openFile3->of_offset, UIO_WRITE);
		result = VOP_WRITE(openFile3->of_vnode, &wUIO);
		if(result != 0)
			return result;

		openFile3->of_offset = wUIO.uio_offset;
		lock_release(openFile3->of_offsetlock);
	
		//increment counter
		counter = counter + 4;
	}

	*retval = openFile3->of_offset;
	
	//closing files

	KASSERT(filetable_okfd(curproc->p_filetable, fd));

	filetable_placeat(curproc->p_filetable, NULL, fd, &openFile1);
	filetable_placeat(curproc->p_filetable, NULL, fd, &openFile2);
	filetable_placeat(curproc->p_filetable, NULL, fd, &openFile3);
	
	if(openFile1 == NULL)
		return ENOENT;
	if(openFile2 == NULL)
		return ENOENT;
	if(openFile3 == NULL)
		return ENOENT;
	
	openfile_decref(openFile1);
	openfile_decref(openFile2);
	openfile_decref(openFile3);

	kfree(kpath_pn1);
	kfree(kpath_pn2);
	kfree(kpath_pn3);
	kfree(buffer1);
	kfree(buffer2);

	return 0;
}
/* 
* meld () - combine the content of two files word by word into a new file
*/
